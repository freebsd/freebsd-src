/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include "rtsock_common.h"
#include "rtsock_config.h"

static inline struct rtsock_test_config *
presetup_ipv6(const atf_tc_t *tc)
{
	struct rtsock_test_config *c;
	int ret;

	c = config_setup(tc);

	ret = iface_turn_up(c->ifname);
	ATF_REQUIRE_MSG(ret == 0, "Unable to turn up %s", c->ifname);

	ret = iface_enable_ipv6(c->ifname);
	ATF_REQUIRE_MSG(ret == 0, "Unable to enable IPv6 on %s", c->ifname);

	ret = iface_setup_addr(c->ifname, c->addr6_str, c->plen6);

	c->rtsock_fd = rtsock_setup_socket();

	return (c);
}

static inline struct rtsock_test_config *
presetup_ipv4(const atf_tc_t *tc)
{
	struct rtsock_test_config *c;
	int ret;

	c = config_setup(tc);

	/* assumes ifconfig doing IFF_UP */
	ret = iface_setup_addr(c->ifname, c->addr4_str, c->plen4);
	ATF_REQUIRE_MSG(ret == 0, "ifconfig failed");

	/* Actually open interface, so kernel writes won't fail */
	if (c->autocreated_interface) {
		ret = iface_open(c->ifname);
		ATF_REQUIRE_MSG(ret >= 0, "unable to open interface %s", c->ifname);
	}

	c->rtsock_fd = rtsock_setup_socket();

	return (c);
}


static void
prepare_v4_network(struct rtsock_test_config *c, struct sockaddr_in *dst,
  struct sockaddr_in *mask, struct sockaddr_in *gw)
{
	/* Create IPv4 subnetwork with smaller prefix */
	sa_fill_mask4(mask, c->plen4 + 1);
	*dst = c->net4;
	/* Calculate GW as last-net-address - 1 */
	*gw = c->net4;
	gw->sin_addr.s_addr = htonl((ntohl(c->net4.sin_addr.s_addr) | ~ntohl(c->mask4.sin_addr.s_addr)) - 1);
	sa_print((struct sockaddr *)dst, 0);
	sa_print((struct sockaddr *)mask, 0);
	sa_print((struct sockaddr *)gw, 0);
}

static void
prepare_v6_network(struct rtsock_test_config *c, struct sockaddr_in6 *dst,
  struct sockaddr_in6 *mask, struct sockaddr_in6 *gw)
{
	/* Create IPv6 subnetwork with smaller prefix */
	sa_fill_mask6(mask, c->plen6 + 1);
	*dst = c->net6;
	/* Calculate GW as last-net-address - 1 */
	*gw = c->net6;
#define _s6_addr32 __u6_addr.__u6_addr32
	gw->sin6_addr._s6_addr32[0] = htonl((ntohl(gw->sin6_addr._s6_addr32[0]) | ~ntohl(c->mask6.sin6_addr._s6_addr32[0])));
	gw->sin6_addr._s6_addr32[1] = htonl((ntohl(gw->sin6_addr._s6_addr32[1]) | ~ntohl(c->mask6.sin6_addr._s6_addr32[1])));
	gw->sin6_addr._s6_addr32[2] = htonl((ntohl(gw->sin6_addr._s6_addr32[2]) | ~ntohl(c->mask6.sin6_addr._s6_addr32[2])));
	gw->sin6_addr._s6_addr32[3] = htonl((ntohl(gw->sin6_addr._s6_addr32[3]) | ~ntohl(c->mask6.sin6_addr._s6_addr32[3])) - 1);
#undef _s6_addr32
	sa_print((struct sockaddr *)dst, 0);
	sa_print((struct sockaddr *)mask, 0);
	sa_print((struct sockaddr *)gw, 0);
}

static void
prepare_route_message(struct rt_msghdr *rtm, int cmd, struct sockaddr *dst,
  struct sockaddr *mask, struct sockaddr *gw)
{

	rtsock_prepare_route_message(rtm, cmd, dst, mask, gw);

	if (cmd == RTM_ADD || cmd == RTM_CHANGE)
		rtm->rtm_flags |= RTF_STATIC;
}

static void
verify_route_message(struct rt_msghdr *rtm, int cmd, struct sockaddr *dst,
  struct sockaddr *mask, struct sockaddr *gw)
{
	char msg[512];
	struct sockaddr *sa;
	int ret;

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_type == cmd,
	    "expected %s message, got %d (%s)", rtsock_print_cmdtype(cmd),
	    rtm->rtm_type, rtsock_print_cmdtype(rtm->rtm_type));
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_errno == 0,
	    "got got errno %d as message reply", rtm->rtm_errno);
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->_rtm_spare1 == 0,
	    "expected rtm_spare==0, got %d", rtm->_rtm_spare1);

	/* kernel MAY return more sockaddrs, including RTA_IFP / RTA_IFA, so verify the needed ones */
	if (dst != NULL) {
		sa = rtsock_find_rtm_sa(rtm, RTA_DST);
		RTSOCK_ATF_REQUIRE_MSG(rtm, sa != NULL, "DST is not set");
		ret = sa_equal_msg(sa, dst, msg, sizeof(msg));
		RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "DST sa diff: %s", msg);
	}

	if (mask != NULL) {
		sa = rtsock_find_rtm_sa(rtm, RTA_NETMASK);
		RTSOCK_ATF_REQUIRE_MSG(rtm, sa != NULL, "NETMASK is not set");
		ret = sa_equal_msg(sa, mask, msg, sizeof(msg));
		RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "NETMASK sa diff: %s", msg);
	}

	if (gw != NULL) {
		sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
		RTSOCK_ATF_REQUIRE_MSG(rtm, sa != NULL, "GATEWAY is not set");
		ret = sa_equal_msg(sa, gw, msg, sizeof(msg));
		RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "GATEWAY sa diff: %s", msg);
	}

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_pid > 0, "expected non-zero pid");
}

static void
verify_route_message_extra(struct rt_msghdr *rtm, int ifindex, int rtm_flags)
{
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_index == ifindex,
	    "expected ifindex %d, got %d", ifindex, rtm->rtm_index);

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_flags == rtm_flags,
	    "expected flags: %X, got %X", rtm_flags, rtm->rtm_flags);
}

/* TESTS */

#define	DECLARE_TEST_VARS					\
	char buffer[2048];					\
	struct rtsock_test_config *c;				\
	struct rt_msghdr *rtm = (struct rt_msghdr *)buffer;	\
	struct sockaddr *sa;					\
	int ret;						\
								\

#define	DESCRIBE_ROOT_TEST(_msg)	config_describe_root_test(tc, _msg)
#define	CLEANUP_AFTER_TEST	config_generic_cleanup(config_setup(tc))


ATF_TC_WITH_CLEANUP(rtm_get_v4_exact_success);
ATF_TC_HEAD(rtm_get_v4_exact_success, tc)
{
	DESCRIBE_ROOT_TEST("Tests RTM_GET with exact prefix lookup on an interface prefix");
}

ATF_TC_BODY(rtm_get_v4_exact_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&c->net4,
	    (struct sockaddr *)&c->mask4, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/*
	 * RTM_GET: Report Metrics: len 240, pid: 45072, seq 42, errno 0, flags: <UP,DONE,PINNED>
	 * sockaddrs: 0x7 <DST,GATEWAY,NETMASK>
	 *  af=inet len=16 addr=192.0.2.0 hd={10, 02, 00{2}, C0, 00, 02, 00{9}}
	 *  af=link len=54 sdl_index=3 if_name=tap4242 hd={36, 12, 03, 00, 06, 00{49}}
	 *  af=inet len=16 addr=255.255.255.0 hd={10, 02, FF{5}, 00{9}}
	 */

	verify_route_message(rtm, RTM_GET, (struct sockaddr *)&c->net4,
	    (struct sockaddr *)&c->mask4, NULL);

	verify_route_message_extra(rtm, c->ifindex, RTF_UP | RTF_DONE | RTF_PINNED);

	/* Explicitly verify gateway for the interface route */
	sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
	RTSOCK_ATF_REQUIRE_MSG(rtm, sa != NULL, "GATEWAY is not set");
	RTSOCK_ATF_REQUIRE_MSG(rtm, sa->sa_family == AF_LINK, "GW sa family is %d", sa->sa_family);
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;
	RTSOCK_ATF_REQUIRE_MSG(rtm, sdl->sdl_index == c->ifindex, "GW ifindex is %d", sdl->sdl_index);
}

ATF_TC_CLEANUP(rtm_get_v4_exact_success, tc)
{
	CLEANUP_AFTER_TEST;
}

ATF_TC_WITH_CLEANUP(rtm_get_v4_lpm_success);
ATF_TC_HEAD(rtm_get_v4_lpm_success, tc)
{
	DESCRIBE_ROOT_TEST("Tests RTM_GET with address lookup on an existing prefix");
}

ATF_TC_BODY(rtm_get_v4_lpm_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&c->net4, NULL, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));

	/*
	 * RTM_GET: Report Metrics: len 312, pid: 67074, seq 1, errno 0, flags:<UP,DONE,PINNED>
	 * locks:  inits:
	 * sockaddrs: <DST,GATEWAY,NETMASK,IFP,IFA>
	 * 10.0.0.0 link#1 255.255.255.0 vtnet0:52.54.0.42.f.ef 10.0.0.157
	 */
	
	verify_route_message(rtm, RTM_GET, (struct sockaddr *)&c->net4,
	    (struct sockaddr *)&c->mask4, NULL);

	verify_route_message_extra(rtm, c->ifindex, RTF_UP | RTF_DONE | RTF_PINNED);
}

ATF_TC_CLEANUP(rtm_get_v4_lpm_success, tc)
{
	CLEANUP_AFTER_TEST;
}


ATF_TC_WITH_CLEANUP(rtm_get_v4_empty_dst_failure);
ATF_TC_HEAD(rtm_get_v4_empty_dst_failure, tc)
{

	DESCRIBE_ROOT_TEST("Tests RTM_GET with empty DST addr");
}

ATF_TC_BODY(rtm_get_v4_empty_dst_failure, tc)
{
	DECLARE_TEST_VARS;

	c = config_setup_base(tc);
	c->rtsock_fd = rtsock_setup_socket();

	rtsock_prepare_route_message(rtm, RTM_GET, NULL,
	    (struct sockaddr *)&c->mask4, NULL);
	rtsock_update_rtm_len(rtm);

	write(c->rtsock_fd, rtm, rtm->rtm_msglen);
	ATF_CHECK_ERRNO(EINVAL, write(c->rtsock_fd, rtm, rtm->rtm_msglen));
}

ATF_TC_CLEANUP(rtm_get_v4_empty_dst_failure, tc)
{
	CLEANUP_AFTER_TEST;
}

ATF_TC_WITH_CLEANUP(rtm_get_v4_hostbits_failure);
ATF_TC_HEAD(rtm_get_v4_hostbits_failure, tc)
{
	DESCRIBE_ROOT_TEST("Tests RTM_GET with prefix with some hosts-bits set");
}

ATF_TC_BODY(rtm_get_v4_hostbits_failure, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	/* Q the same prefix */
	rtsock_prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&c->addr4,
	    (struct sockaddr *)&c->mask4, NULL);
	rtsock_update_rtm_len(rtm);

	ATF_CHECK_ERRNO(ESRCH, write(c->rtsock_fd, rtm, rtm->rtm_msglen));
}

ATF_TC_CLEANUP(rtm_get_v4_hostbits_failure, tc)
{
	CLEANUP_AFTER_TEST;
}

ATF_TC_WITH_CLEANUP(rtm_add_v4_gw_direct_success);
ATF_TC_HEAD(rtm_add_v4_gw_direct_success, tc)
{
	DESCRIBE_ROOT_TEST("Tests IPv4 route addition with directly-reachable GW specified by IP");
}

ATF_TC_BODY(rtm_add_v4_gw_direct_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	/* Create IPv4 subnetwork with smaller prefix */
	struct sockaddr_in mask4;
	struct sockaddr_in net4;
	struct sockaddr_in gw4;
	prepare_v4_network(c, &net4, &mask4, &gw4);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/*
	 * RTM_ADD: Add Route: len 200, pid: 46068, seq 42, errno 0, flags:<GATEWAY,DONE,STATIC>
	 * locks:  inits:
	 * sockaddrs: <DST,GATEWAY,NETMASK>
	 *  192.0.2.0 192.0.2.254 255.255.255.128
	 */

	verify_route_message(rtm, RTM_ADD, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);
	/* XXX: Currently kernel sets RTF_UP automatically but does NOT report it in the reply */
	verify_route_message_extra(rtm, c->ifindex, RTF_DONE | RTF_GATEWAY | RTF_STATIC);
}

ATF_TC_CLEANUP(rtm_add_v4_gw_direct_success, tc)
{
	CLEANUP_AFTER_TEST;
}

ATF_TC_WITH_CLEANUP(rtm_del_v4_prefix_nogw_success);
ATF_TC_HEAD(rtm_del_v4_prefix_nogw_success, tc)
{
	DESCRIBE_ROOT_TEST("Tests IPv4 route removal without specifying gateway");
}

ATF_TC_BODY(rtm_del_v4_prefix_nogw_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	/* Create IPv4 subnetwork with smaller prefix */
	struct sockaddr_in mask4;
	struct sockaddr_in net4;
	struct sockaddr_in gw4;
	prepare_v4_network(c, &net4, &mask4, &gw4);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	/* Route has been added successfully, try to delete it */
	prepare_route_message(rtm, RTM_DELETE, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/*
	 * RTM_DELETE: Delete Route: len 200, pid: 46417, seq 43, errno 0, flags: <GATEWAY,DONE,STATIC>
	 * sockaddrs: 0x7 <DST,GATEWAY,NETMASK>
	 *  af=inet len=16 addr=192.0.2.0 hd={10, 02, 00{2}, C0, 00, 02, 00{9}}
	 *  af=inet len=16 addr=192.0.2.254 hd={10, 02, 00{2}, C0, 00, 02, FE, 00{8}}
	 *  af=inet len=16 addr=255.255.255.128 hd={10, 02, FF{5}, 80, 00{8}}
	 */
	verify_route_message(rtm, RTM_DELETE, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);

	verify_route_message_extra(rtm, c->ifindex, RTF_DONE | RTF_GATEWAY | RTF_STATIC);
}

ATF_TC_CLEANUP(rtm_del_v4_prefix_nogw_success, tc)
{
	CLEANUP_AFTER_TEST;
}

ATF_TC_WITH_CLEANUP(rtm_add_v6_gu_gw_gu_direct_success);
ATF_TC_HEAD(rtm_add_v6_gu_gw_gu_direct_success, tc)
{
	DESCRIBE_ROOT_TEST("Tests IPv6 global unicast prefix addition with directly-reachable GU GW");
}

ATF_TC_BODY(rtm_add_v6_gu_gw_gu_direct_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6(tc);

	/* Create IPv6 subnetwork with smaller prefix */
	struct sockaddr_in6 mask6;
	struct sockaddr_in6 net6;
	struct sockaddr_in6 gw6;
	prepare_v6_network(c, &net6, &mask6, &gw6);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/*
	 * RTM_ADD: Add Route: len 200, pid: 46068, seq 42, errno 0, flags:<GATEWAY,DONE,STATIC>
	 * locks:  inits:
	 * sockaddrs: <DST,GATEWAY,NETMASK>
	 *  192.0.2.0 192.0.2.254 255.255.255.128
	 */

	verify_route_message(rtm, RTM_ADD, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	/* XXX: Currently kernel sets RTF_UP automatically but does NOT report it in the reply */
	verify_route_message_extra(rtm, c->ifindex, RTF_DONE | RTF_GATEWAY | RTF_STATIC);
}

ATF_TC_CLEANUP(rtm_add_v6_gu_gw_gu_direct_success, tc)
{
	CLEANUP_AFTER_TEST;
}

ATF_TC_WITH_CLEANUP(rtm_del_v6_gu_prefix_nogw_success);
ATF_TC_HEAD(rtm_del_v6_gu_prefix_nogw_success, tc)
{

	DESCRIBE_ROOT_TEST("Tests IPv6 global unicast prefix removal without specifying gateway");
}

ATF_TC_BODY(rtm_del_v6_gu_prefix_nogw_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6(tc);

	/* Create IPv6 subnetwork with smaller prefix */
	struct sockaddr_in6 mask6;
	struct sockaddr_in6 net6;
	struct sockaddr_in6 gw6;
	prepare_v6_network(c, &net6, &mask6, &gw6);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	/* Route has been added successfully, try to delete it */
	prepare_route_message(rtm, RTM_DELETE, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/*
	 * RTM_DELETE: Delete Route: len 200, pid: 46417, seq 43, errno 0, flags: <GATEWAY,DONE,STATIC>
	 * sockaddrs: 0x7 <DST,GATEWAY,NETMASK>
	 *  af=inet len=16 addr=192.0.2.0 hd={10, 02, 00{2}, C0, 00, 02, 00{9}}
	 *  af=inet len=16 addr=192.0.2.254 hd={10, 02, 00{2}, C0, 00, 02, FE, 00{8}}
	 *  af=inet len=16 addr=255.255.255.128 hd={10, 02, FF{5}, 80, 00{8}}
	 */

	verify_route_message(rtm, RTM_DELETE, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);
	verify_route_message_extra(rtm, c->ifindex, RTF_DONE | RTF_GATEWAY | RTF_STATIC);
}

ATF_TC_CLEANUP(rtm_del_v6_gu_prefix_nogw_success, tc)
{
	CLEANUP_AFTER_TEST;
}



ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, rtm_get_v4_exact_success);
	ATF_TP_ADD_TC(tp, rtm_get_v4_lpm_success);
	ATF_TP_ADD_TC(tp, rtm_get_v4_hostbits_failure);
	ATF_TP_ADD_TC(tp, rtm_get_v4_empty_dst_failure);
	ATF_TP_ADD_TC(tp, rtm_add_v4_gw_direct_success);
	ATF_TP_ADD_TC(tp, rtm_del_v4_prefix_nogw_success);
	ATF_TP_ADD_TC(tp, rtm_add_v6_gu_gw_gu_direct_success);
	ATF_TP_ADD_TC(tp, rtm_del_v6_gu_prefix_nogw_success);

	return (atf_no_error());
}

