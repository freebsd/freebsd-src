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

#include "rtsock_common.h"
#include "rtsock_config.h"
#include "sys/types.h"
#include <sys/time.h>
#include <sys/ioctl.h>

#include "net/bpf.h"

static void
jump_vnet(struct rtsock_test_config *c, const atf_tc_t *tc)
{
	char vnet_name[512];

	snprintf(vnet_name, sizeof(vnet_name), "vt-%s", atf_tc_get_ident(tc));
	RLOG("jumping to %s", vnet_name);

	vnet_switch(vnet_name, c->ifnames, c->num_interfaces);

	/* Update ifindex cache */
	c->ifindex = if_nametoindex(c->ifname);
}

static inline struct rtsock_test_config *
presetup_ipv6_iface(const atf_tc_t *tc)
{
	struct rtsock_test_config *c;
	int ret;

	c = config_setup(tc, NULL);

	jump_vnet(c, tc);

	ret = iface_turn_up(c->ifname);
	ATF_REQUIRE_MSG(ret == 0, "Unable to turn up %s", c->ifname);

	ret = iface_enable_ipv6(c->ifname);
	ATF_REQUIRE_MSG(ret == 0, "Unable to enable IPv6 on %s", c->ifname);
	ATF_REQUIRE_ERRNO(0, true);

	return (c);
}

static inline struct rtsock_test_config *
presetup_ipv6(const atf_tc_t *tc)
{
	struct rtsock_test_config *c;
	int ret;

	c = presetup_ipv6_iface(tc);

	ret = iface_setup_addr(c->ifname, c->addr6_str, c->plen6);

	c->rtsock_fd = rtsock_setup_socket();
	ATF_REQUIRE_ERRNO(0, true);

	return (c);
}

static inline struct rtsock_test_config *
presetup_ipv4_iface(const atf_tc_t *tc)
{
	struct rtsock_test_config *c;
	int ret;

	c = config_setup(tc, NULL);
	ATF_REQUIRE(c != NULL);

	jump_vnet(c, tc);

	ret = iface_turn_up(c->ifname);
	ATF_REQUIRE_MSG(ret == 0, "Unable to turn up %s", c->ifname);
	ATF_REQUIRE_ERRNO(0, true);

	return (c);
}

static inline struct rtsock_test_config *
presetup_ipv4(const atf_tc_t *tc)
{
	struct rtsock_test_config *c;
	int ret;

	c = presetup_ipv4_iface(tc);

	/* assumes ifconfig doing IFF_UP */
	ret = iface_setup_addr(c->ifname, c->addr4_str, c->plen4);
	ATF_REQUIRE_MSG(ret == 0, "ifconfig failed");

	c->rtsock_fd = rtsock_setup_socket();
	ATF_REQUIRE_ERRNO(0, true);

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
		ret = 1;
		RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "NETMASK sa diff: %s", msg);
	}

	if (gw != NULL) {
		sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
		RTSOCK_ATF_REQUIRE_MSG(rtm, sa != NULL, "GATEWAY is not set");
		ret = sa_equal_msg(sa, gw, msg, sizeof(msg));
		RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "GATEWAY sa diff: %s", msg);
	}
}

static void
verify_route_message_extra(struct rt_msghdr *rtm, int ifindex, int rtm_flags)
{
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_index == ifindex,
	    "expected ifindex %d, got %d", ifindex, rtm->rtm_index);

	if (rtm->rtm_flags != rtm_flags) {
		char got_flags[64], expected_flags[64];
		rtsock_print_rtm_flags(got_flags, sizeof(got_flags),
		    rtm->rtm_flags);
		rtsock_print_rtm_flags(expected_flags, sizeof(expected_flags),
		    rtm_flags);

		RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_flags == rtm_flags,
		    "expected flags: 0x%X %s, got 0x%X %s",
		    rtm_flags, expected_flags,
		    rtm->rtm_flags, got_flags);
	}
}

static void
verify_link_gateway(struct rt_msghdr *rtm, int ifindex)
{
	struct sockaddr *sa;
	struct sockaddr_dl *sdl;

	sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
	RTSOCK_ATF_REQUIRE_MSG(rtm, sa != NULL, "GATEWAY is not set");
	RTSOCK_ATF_REQUIRE_MSG(rtm, sa->sa_family == AF_LINK, "GW sa family is %d", sa->sa_family);
	sdl = (struct sockaddr_dl *)sa;
	RTSOCK_ATF_REQUIRE_MSG(rtm, sdl->sdl_index == ifindex, "GW ifindex is %d", sdl->sdl_index);
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
#define	CLEANUP_AFTER_TEST	config_generic_cleanup(tc)

#define	RTM_DECLARE_ROOT_TEST(_name, _descr)			\
ATF_TC_WITH_CLEANUP(_name);					\
ATF_TC_HEAD(_name, tc)						\
{								\
	DESCRIBE_ROOT_TEST(_descr);				\
}								\
ATF_TC_CLEANUP(_name, tc)					\
{								\
	CLEANUP_AFTER_TEST;					\
}

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
	verify_link_gateway(rtm, c->ifindex);
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

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

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
	struct rtsock_config_options co;

	bzero(&co, sizeof(co));
	co.num_interfaces = 0;
	
	c = config_setup(tc,&co);
	c->rtsock_fd = rtsock_setup_socket();

	rtsock_prepare_route_message(rtm, RTM_GET, NULL,
	    (struct sockaddr *)&c->mask4, NULL);
	rtsock_update_rtm_len(rtm);

	ATF_CHECK_ERRNO(EINVAL, write(c->rtsock_fd, rtm, rtm->rtm_msglen) == -1);
}

ATF_TC_CLEANUP(rtm_get_v4_empty_dst_failure, tc)
{
	CLEANUP_AFTER_TEST;
}

ATF_TC_WITH_CLEANUP(rtm_get_v4_hostbits_success);
ATF_TC_HEAD(rtm_get_v4_hostbits_success, tc)
{
	DESCRIBE_ROOT_TEST("Tests RTM_GET with prefix with some hosts-bits set");
}

ATF_TC_BODY(rtm_get_v4_hostbits_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	/* Q the same prefix */
	rtsock_prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&c->addr4,
	    (struct sockaddr *)&c->mask4, NULL);
	rtsock_update_rtm_len(rtm);

	ATF_REQUIRE_ERRNO(0, true);
	ATF_CHECK_ERRNO(0, write(c->rtsock_fd, rtm, rtm->rtm_msglen) > 0);
}

ATF_TC_CLEANUP(rtm_get_v4_hostbits_success, tc)
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
	verify_route_message_extra(rtm, c->ifindex,
	    RTF_UP | RTF_DONE | RTF_GATEWAY | RTF_STATIC);
}

ATF_TC_CLEANUP(rtm_add_v4_gw_direct_success, tc)
{
	CLEANUP_AFTER_TEST;
}

RTM_DECLARE_ROOT_TEST(rtm_add_v4_no_rtf_host_success,
    "Tests success with netmask sa and RTF_HOST inconsistency");

ATF_TC_BODY(rtm_add_v4_no_rtf_host_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	/* Create IPv4 subnetwork with smaller prefix */
	struct sockaddr_in mask4;
	struct sockaddr_in net4;
	struct sockaddr_in gw4;
	prepare_v4_network(c, &net4, &mask4, &gw4);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net4,
	    NULL, (struct sockaddr *)&gw4);
	rtsock_update_rtm_len(rtm);

	/* RTF_HOST is NOT specified, while netmask is empty */
	ATF_REQUIRE_ERRNO(0, true);
	ATF_CHECK_ERRNO(0, write(c->rtsock_fd, rtm, rtm->rtm_msglen) > 0);
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

RTM_DECLARE_ROOT_TEST(rtm_change_v4_gw_success,
    "Tests IPv4 gateway change");

ATF_TC_BODY(rtm_change_v4_gw_success, tc)
{
	DECLARE_TEST_VARS;
	struct rtsock_config_options co;

	bzero(&co, sizeof(co));
	co.num_interfaces = 2;

	c = config_setup(tc, &co);
	jump_vnet(c, tc);

	ret = iface_turn_up(c->ifnames[0]);
	ATF_REQUIRE_MSG(ret == 0, "Unable to turn up %s", c->ifnames[0]);
	ret = iface_turn_up(c->ifnames[1]);
	ATF_REQUIRE_MSG(ret == 0, "Unable to turn up %s", c->ifnames[1]);

	ret = iface_setup_addr(c->ifnames[0], c->addr4_str, c->plen4);
	ATF_REQUIRE_MSG(ret == 0, "ifconfig failed");

	/* Use 198.51.100.0/24 "TEST-NET-2" for the second interface */
	ret = iface_setup_addr(c->ifnames[1], "198.51.100.1", 24);
	ATF_REQUIRE_MSG(ret == 0, "ifconfig failed");

	c->rtsock_fd = rtsock_setup_socket();

	/* Create IPv4 subnetwork with smaller prefix */
	struct sockaddr_in mask4;
	struct sockaddr_in net4;
	struct sockaddr_in gw4;
	prepare_v4_network(c, &net4, &mask4, &gw4);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	verify_route_message(rtm, RTM_ADD, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);

	/* Change gateway to the one on desiding on the other interface */
	inet_pton(AF_INET, "198.51.100.2", &gw4.sin_addr.s_addr);
	prepare_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);
	rtsock_send_rtm(c->rtsock_fd, rtm);

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	verify_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);

	verify_route_message_extra(rtm, if_nametoindex(c->ifnames[1]),
	    RTF_UP | RTF_DONE | RTF_GATEWAY | RTF_STATIC);

	/* Verify the change has actually taken place */
	prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	/*
	 * RTM_GET: len 200, pid: 3894, seq 44, errno 0, flags: <UP,GATEWAY,DONE,STATIC>
	 *  sockaddrs: 0x7 <DST,GATEWAY,NETMASK>
 	 *  af=inet len=16 addr=192.0.2.0 hd={x10, x02, x00{2}, xC0, x00, x02, x00{9}}
	 *  af=inet len=16 addr=198.51.100.2 hd={x10, x02, x00{2}, xC6, x33, x64, x02, x00{8}}
	 *  af=inet len=16 addr=255.255.255.128 hd={x10, x02, xFF, xFF, xFF, xFF, xFF, x80, x00{8}}
	 */

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);
	verify_route_message_extra(rtm, if_nametoindex(c->ifnames[1]),
	    RTF_UP | RTF_DONE | RTF_GATEWAY | RTF_STATIC);

}

RTM_DECLARE_ROOT_TEST(rtm_change_v4_mtu_success,
    "Tests IPv4 path mtu change");

ATF_TC_BODY(rtm_change_v4_mtu_success, tc)
{
	DECLARE_TEST_VARS;

	unsigned long test_mtu = 1442;

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

	/* Change MTU */
	prepare_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, NULL);
	rtm->rtm_inits |= RTV_MTU;
	rtm->rtm_rmx.rmx_mtu = test_mtu;

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	verify_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, NULL);

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_rmx.rmx_mtu == test_mtu,
	    "expected mtu: %lu, got %lu", test_mtu, rtm->rtm_rmx.rmx_mtu);

	/* Verify the change has actually taken place */
	prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_rmx.rmx_mtu == test_mtu,
	    "expected mtu: %lu, got %lu", test_mtu, rtm->rtm_rmx.rmx_mtu);
}

RTM_DECLARE_ROOT_TEST(rtm_change_v4_flags_success,
    "Tests IPv4 path flags change");

ATF_TC_BODY(rtm_change_v4_flags_success, tc)
{
	DECLARE_TEST_VARS;

	uint32_t test_flags = RTF_PROTO1 | RTF_PROTO2 | RTF_PROTO3 | RTF_STATIC;
	uint32_t desired_flags;

	c = presetup_ipv4(tc);

	/* Create IPv4 subnetwork with smaller prefix */
	struct sockaddr_in mask4;
	struct sockaddr_in net4;
	struct sockaddr_in gw4;
	prepare_v4_network(c, &net4, &mask4, &gw4);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);

	/* Set test flags during route addition */
	desired_flags = RTF_UP | RTF_DONE | RTF_GATEWAY | test_flags;
	rtm->rtm_flags |= test_flags;
	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/* Change flags */
	prepare_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, NULL);
	rtm->rtm_flags &= ~test_flags;
	desired_flags &= ~test_flags;

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/* Verify updated flags */
	verify_route_message_extra(rtm, c->ifindex, desired_flags | RTF_DONE);

	/* Verify the change has actually taken place */
	prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	verify_route_message_extra(rtm, c->ifindex, desired_flags | RTF_DONE);
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

	verify_route_message_extra(rtm, c->ifindex,
	    RTF_UP | RTF_DONE | RTF_GATEWAY | RTF_STATIC);
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

RTM_DECLARE_ROOT_TEST(rtm_change_v6_gw_success,
    "Tests IPv6 gateway change");

ATF_TC_BODY(rtm_change_v6_gw_success, tc)
{
	DECLARE_TEST_VARS;
	struct rtsock_config_options co;

	bzero(&co, sizeof(co));
	co.num_interfaces = 2;

	c = config_setup(tc, &co);
	jump_vnet(c, tc);

	ret = iface_turn_up(c->ifnames[0]);
	ATF_REQUIRE_MSG(ret == 0, "Unable to turn up %s", c->ifnames[0]);
	ret = iface_turn_up(c->ifnames[1]);
	ATF_REQUIRE_MSG(ret == 0, "Unable to turn up %s", c->ifnames[1]);

	ret = iface_enable_ipv6(c->ifnames[0]);
	ATF_REQUIRE_MSG(ret == 0, "Unable to enable IPv6 on %s", c->ifnames[0]);
	ret = iface_enable_ipv6(c->ifnames[1]);
	ATF_REQUIRE_MSG(ret == 0, "Unable to enable IPv6 on %s", c->ifnames[1]);

	ret = iface_setup_addr(c->ifnames[0], c->addr6_str, c->plen6);
	ATF_REQUIRE_MSG(ret == 0, "ifconfig failed");

	ret = iface_setup_addr(c->ifnames[1], "2001:DB8:4242::1", 64);
	ATF_REQUIRE_MSG(ret == 0, "ifconfig failed");

	c->rtsock_fd = rtsock_setup_socket();

	/* Create IPv6 subnetwork with smaller prefix */
	struct sockaddr_in6 mask6;
	struct sockaddr_in6 net6;
	struct sockaddr_in6 gw6;
	prepare_v6_network(c, &net6, &mask6, &gw6);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	verify_route_message(rtm, RTM_ADD, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	/* Change gateway to the one on residing on the other interface */
	inet_pton(AF_INET6, "2001:DB8:4242::4242", &gw6.sin6_addr);
	prepare_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);
	rtsock_send_rtm(c->rtsock_fd, rtm);

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	verify_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	verify_route_message_extra(rtm, if_nametoindex(c->ifnames[1]),
	    RTF_UP | RTF_DONE | RTF_GATEWAY | RTF_STATIC);

	/* Verify the change has actually taken place */
	prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	/*
	 * RTM_GET: len 248, pid: 2268, seq 44, errno 0, flags: <UP,GATEWAY,DONE,STATIC>
	 *  sockaddrs: 0x7 <DST,GATEWAY,NETMASK>
	 *  af=inet6 len=28 addr=2001:db8:: hd={x1C, x1C, x00{6}, x20, x01, x0D, xB8, x00{16}}
	 *  af=inet6 len=28 addr=2001:db8:4242::4242 hd={x1C, x1C, x00{6}, x20, x01, x0D, xB8, x42, x42, x00{8}, x42, x42, x00{4}}
	 *  af=inet6 len=28 addr=ffff:ffff:8000:: hd={x1C, x1C, xFF, xFF, xFF, xFF, xFF, xFF, xFF, xFF, xFF, xFF, x80, x00{15}}
	 */

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);
	verify_route_message_extra(rtm, if_nametoindex(c->ifnames[1]),
	    RTF_UP | RTF_DONE | RTF_GATEWAY | RTF_STATIC);
}

RTM_DECLARE_ROOT_TEST(rtm_change_v6_mtu_success,
    "Tests IPv6 path mtu change");

ATF_TC_BODY(rtm_change_v6_mtu_success, tc)
{
	DECLARE_TEST_VARS;

	unsigned long test_mtu = 1442;

	c = presetup_ipv6(tc);

	/* Create IPv6 subnetwork with smaller prefix */
	struct sockaddr_in6 mask6;
	struct sockaddr_in6 net6;
	struct sockaddr_in6 gw6;
	prepare_v6_network(c, &net6, &mask6, &gw6);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	/* Send route add */
	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/* Change MTU */
	prepare_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, NULL);
	rtm->rtm_inits |= RTV_MTU;
	rtm->rtm_rmx.rmx_mtu = test_mtu;

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	verify_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, NULL);

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_rmx.rmx_mtu == test_mtu,
	    "expected mtu: %lu, got %lu", test_mtu, rtm->rtm_rmx.rmx_mtu);

	/* Verify the change has actually taken place */
	prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_rmx.rmx_mtu == test_mtu,
	    "expected mtu: %lu, got %lu", test_mtu, rtm->rtm_rmx.rmx_mtu);
}

RTM_DECLARE_ROOT_TEST(rtm_change_v6_flags_success,
    "Tests IPv6 path flags change");

ATF_TC_BODY(rtm_change_v6_flags_success, tc)
{
	DECLARE_TEST_VARS;

	uint32_t test_flags = RTF_PROTO1 | RTF_PROTO2 | RTF_PROTO3 | RTF_STATIC;
	uint32_t desired_flags;

	c = presetup_ipv6(tc);

	/* Create IPv6 subnetwork with smaller prefix */
	struct sockaddr_in6 mask6;
	struct sockaddr_in6 net6;
	struct sockaddr_in6 gw6;
	prepare_v6_network(c, &net6, &mask6, &gw6);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	/* Set test flags during route addition */
	desired_flags = RTF_UP | RTF_DONE | RTF_GATEWAY | test_flags;
	rtm->rtm_flags |= test_flags;
	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/* Change flags */
	prepare_route_message(rtm, RTM_CHANGE, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, NULL);
	rtm->rtm_flags &= ~test_flags;
	desired_flags &= ~test_flags;

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	/* Verify updated flags */
	verify_route_message_extra(rtm, c->ifindex, desired_flags | RTF_DONE);

	/* Verify the change has actually taken place */
	prepare_route_message(rtm, RTM_GET, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, NULL);

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	verify_route_message_extra(rtm, c->ifindex, desired_flags | RTF_DONE);
}

ATF_TC_WITH_CLEANUP(rtm_add_v4_temporal1_success);
ATF_TC_HEAD(rtm_add_v4_temporal1_success, tc)
{
	DESCRIBE_ROOT_TEST("Tests IPv4 route expiration with expire time set");
}

ATF_TC_BODY(rtm_add_v4_temporal1_success, tc)
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

	/* Set expire time to now */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	rtm->rtm_rmx.rmx_expire = tv.tv_sec - 1;
	rtm->rtm_inits |= RTV_EXPIRE;

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);
	ATF_REQUIRE_MSG(rtm != NULL, "unable to get rtsock reply for RTM_ADD");
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_inits & RTV_EXPIRE, "RTV_EXPIRE not set");

	/* The next should be route deletion */
	rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));

	verify_route_message(rtm, RTM_DELETE, (struct sockaddr *)&net4,
	    (struct sockaddr *)&mask4, (struct sockaddr *)&gw4);

	verify_route_message_extra(rtm, c->ifindex,
	    RTF_DONE | RTF_GATEWAY | RTF_STATIC);
}

ATF_TC_CLEANUP(rtm_add_v4_temporal1_success, tc)
{
	CLEANUP_AFTER_TEST;
}

ATF_TC_WITH_CLEANUP(rtm_add_v6_temporal1_success);
ATF_TC_HEAD(rtm_add_v6_temporal1_success, tc)
{
	DESCRIBE_ROOT_TEST("Tests IPv6 global unicast prefix addition with directly-reachable GU GW");
}

ATF_TC_BODY(rtm_add_v6_temporal1_success, tc)
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

	/* Set expire time to now */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	rtm->rtm_rmx.rmx_expire = tv.tv_sec - 1;
	rtm->rtm_inits |= RTV_EXPIRE;

	rtsock_send_rtm(c->rtsock_fd, rtm);
	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);
	ATF_REQUIRE_MSG(rtm != NULL, "unable to get rtsock reply for RTM_ADD");
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_inits & RTV_EXPIRE, "RTV_EXPIRE not set");

	/* The next should be route deletion */
	rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));

	verify_route_message(rtm, RTM_DELETE, (struct sockaddr *)&net6,
	    (struct sockaddr *)&mask6, (struct sockaddr *)&gw6);

	verify_route_message_extra(rtm, c->ifindex,
	    RTF_DONE | RTF_GATEWAY | RTF_STATIC);
}

ATF_TC_CLEANUP(rtm_add_v6_temporal1_success, tc)
{
	CLEANUP_AFTER_TEST;
}

/* Interface address messages tests */

RTM_DECLARE_ROOT_TEST(rtm_add_v6_gu_ifa_hostroute_success,
    "Tests validness for /128 host route announce after ifaddr assignment");

ATF_TC_BODY(rtm_add_v6_gu_ifa_hostroute_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6_iface(tc);

	c->rtsock_fd = rtsock_setup_socket();

	ret = iface_setup_addr(c->ifname, c->addr6_str, c->plen6);

	/*
	 * There will be multiple.
	 * RTM_ADD without llinfo.
	 */

	while (true) {
		rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));
		if ((rtm->rtm_type == RTM_ADD) && ((rtm->rtm_flags & RTF_LLINFO) == 0))
			break;
	}
	/* This should be a message for the host route */

	verify_route_message(rtm, RTM_ADD, (struct sockaddr *)&c->addr6, NULL, NULL);
	rtsock_validate_pid_kernel(rtm);
	/* No netmask should be set */
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtsock_find_rtm_sa(rtm, RTA_NETMASK) == NULL, "netmask is set");

	/* gateway should be link sdl with ifindex of an address interface */
	verify_link_gateway(rtm, c->ifindex);

	int expected_rt_flags = RTF_UP | RTF_HOST | RTF_DONE | RTF_STATIC | RTF_PINNED;
	verify_route_message_extra(rtm, if_nametoindex("lo0"), expected_rt_flags);
}

RTM_DECLARE_ROOT_TEST(rtm_add_v6_gu_ifa_prefixroute_success,
    "Tests validness for the prefix route announce after ifaddr assignment");

ATF_TC_BODY(rtm_add_v6_gu_ifa_prefixroute_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6_iface(tc);

	c->rtsock_fd = rtsock_setup_socket();

	ret = iface_setup_addr(c->ifname, c->addr6_str, c->plen6);

	/*
	 * Multiple RTM_ADD messages will be generated:
	 * 1) lladdr mapping (RTF_LLDATA)
	 * 2) host route (one w/o netmask)
	 * 3) prefix route
	 */

	while (true) {
		rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));
		/* Find RTM_ADD with netmask - this should skip both host route and LLADDR */
		if ((rtm->rtm_type == RTM_ADD) && (rtsock_find_rtm_sa(rtm, RTA_NETMASK)))
			break;
	}

	/* This should be a message for the prefix route */
	verify_route_message(rtm, RTM_ADD, (struct sockaddr *)&c->net6,
	    (struct sockaddr *)&c->mask6, NULL);

	/* gateway should be link sdl with ifindex of an address interface */
	verify_link_gateway(rtm, c->ifindex);

	int expected_rt_flags = RTF_UP | RTF_DONE | RTF_PINNED;
	verify_route_message_extra(rtm, c->ifindex, expected_rt_flags);
}

RTM_DECLARE_ROOT_TEST(rtm_add_v6_gu_ifa_ordered_success,
    "Tests ordering of the messages for IPv6 global unicast ifaddr assignment");

ATF_TC_BODY(rtm_add_v6_gu_ifa_ordered_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6_iface(tc);

	c->rtsock_fd = rtsock_setup_socket();

	ret = iface_setup_addr(c->ifname, c->addr6_str, c->plen6);

	int count = 0, tries = 0;

	enum msgtype {
		MSG_IFADDR,
		MSG_HOSTROUTE,
		MSG_PREFIXROUTE,
		MSG_MAX,
	};

	int msg_array[MSG_MAX];

	bzero(msg_array, sizeof(msg_array));

	while (count < 3 && tries < 20) {
		rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));
		tries++;
		/* Classify */
		if (rtm->rtm_type == RTM_NEWADDR) {
			RLOG("MSG_IFADDR: %d", count);
			msg_array[MSG_IFADDR] = count++;
			continue;
		}

		/* Find RTM_ADD with netmask - this should skip both host route and LLADDR */
		if ((rtm->rtm_type == RTM_ADD) && (rtsock_find_rtm_sa(rtm, RTA_NETMASK))) {
			RLOG("MSG_PREFIXROUTE: %d", count);
			msg_array[MSG_PREFIXROUTE] = count++;
			continue;
		}

		if ((rtm->rtm_type == RTM_ADD) && ((rtm->rtm_flags & RTF_LLDATA) == 0)) {
			RLOG("MSG_HOSTROUTE: %d", count);
			msg_array[MSG_HOSTROUTE] = count++;
			continue;
		}

		RLOG("skipping msg type %s, try: %d", rtsock_print_cmdtype(rtm->rtm_type),
		    tries);
	}

	/* TODO: verify multicast */
	ATF_REQUIRE_MSG(count == 3, "Received only %d/3 messages", count);
	ATF_REQUIRE_MSG(msg_array[MSG_IFADDR] == 0, "ifaddr message is not the first");
}

RTM_DECLARE_ROOT_TEST(rtm_del_v6_gu_ifa_hostroute_success,
    "Tests validness for /128 host route removal after ifaddr removal");

ATF_TC_BODY(rtm_del_v6_gu_ifa_hostroute_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6_iface(tc);

	ret = iface_setup_addr(c->ifname, c->addr6_str, c->plen6);

	c->rtsock_fd = rtsock_setup_socket();

	ret = iface_delete_addr(c->ifname, c->addr6_str);

	while (true) {
		rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));
		if ((rtm->rtm_type == RTM_DELETE) &&
		    ((rtm->rtm_flags & RTF_LLINFO) == 0) &&
		    rtsock_find_rtm_sa(rtm, RTA_NETMASK) == NULL)
			break;
	}
	/* This should be a message for the host route */

	verify_route_message(rtm, RTM_DELETE, (struct sockaddr *)&c->addr6, NULL, NULL);
	rtsock_validate_pid_kernel(rtm);
	/* No netmask should be set */
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtsock_find_rtm_sa(rtm, RTA_NETMASK) == NULL, "netmask is set");

	/* gateway should be link sdl with ifindex of an address interface */
	verify_link_gateway(rtm, c->ifindex);

	/* XXX: consider passing ifindex in rtm_index as done in RTM_ADD. */
	int expected_rt_flags = RTF_HOST | RTF_DONE | RTF_STATIC | RTF_PINNED;
	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_flags == expected_rt_flags,
	    "expected rtm flags: 0x%X, got 0x%X", expected_rt_flags, rtm->rtm_flags);
}

RTM_DECLARE_ROOT_TEST(rtm_del_v6_gu_ifa_prefixroute_success,
    "Tests validness for the prefix route removal after ifaddr assignment");

ATF_TC_BODY(rtm_del_v6_gu_ifa_prefixroute_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6_iface(tc);

	ret = iface_setup_addr(c->ifname, c->addr6_str, c->plen6);

	c->rtsock_fd = rtsock_setup_socket();

	ret = iface_delete_addr(c->ifname, c->addr6_str);

	while (true) {
		rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));
		/* Find RTM_DELETE with netmask - this should skip both host route and LLADDR */
		if ((rtm->rtm_type == RTM_DELETE) && (rtsock_find_rtm_sa(rtm, RTA_NETMASK)))
			break;
	}

	/* This should be a message for the prefix route */
	verify_route_message(rtm, RTM_DELETE, (struct sockaddr *)&c->net6,
	    (struct sockaddr *)&c->mask6, NULL);

	/* gateway should be link sdl with ifindex of an address interface */
	verify_link_gateway(rtm, c->ifindex);

	int expected_rt_flags = RTF_DONE | RTF_PINNED;
	verify_route_message_extra(rtm, c->ifindex, expected_rt_flags);
}

RTM_DECLARE_ROOT_TEST(rtm_add_v4_gu_ifa_prefixroute_success,
    "Tests validness for the prefix route announce after ifaddr assignment");

ATF_TC_BODY(rtm_add_v4_gu_ifa_prefixroute_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4_iface(tc);

	c->rtsock_fd = rtsock_setup_socket();

	ret = iface_setup_addr(c->ifname, c->addr6_str, c->plen6);

	/*
	 * Multiple RTM_ADD messages will be generated:
	 * 1) lladdr mapping (RTF_LLDATA)
	 * 3) prefix route
	 */

	while (true) {
		rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));
		/* Find RTM_ADD with netmask - this should skip both host route and LLADDR */
		if ((rtm->rtm_type == RTM_ADD) && (rtsock_find_rtm_sa(rtm, RTA_NETMASK)))
			break;
	}

	/* This should be a message for the prefix route */
	verify_route_message(rtm, RTM_ADD, (struct sockaddr *)&c->net4,
	    (struct sockaddr *)&c->mask4, NULL);

	/* gateway should be link sdl with ifindex of an address interface */
	verify_link_gateway(rtm, c->ifindex);

	int expected_rt_flags = RTF_UP | RTF_DONE | RTF_PINNED;
	verify_route_message_extra(rtm, c->ifindex, expected_rt_flags);
}

RTM_DECLARE_ROOT_TEST(rtm_add_v4_gu_ifa_ordered_success,
    "Tests ordering of the messages for IPv4 unicast ifaddr assignment");

ATF_TC_BODY(rtm_add_v4_gu_ifa_ordered_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4_iface(tc);

	c->rtsock_fd = rtsock_setup_socket();

	ret = iface_setup_addr(c->ifname, c->addr4_str, c->plen4);

	int count = 0, tries = 0;

	enum msgtype {
		MSG_IFADDR,
		MSG_PREFIXROUTE,
		MSG_MAX,
	};

	int msg_array[MSG_MAX];

	bzero(msg_array, sizeof(msg_array));

	while (count < 2 && tries < 20) {
		rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));
		tries++;
		/* Classify */
		if (rtm->rtm_type == RTM_NEWADDR) {
			RLOG("MSG_IFADDR: %d", count);
			msg_array[MSG_IFADDR] = count++;
			continue;
		}

		/* Find RTM_ADD with netmask - this should skip both host route and LLADDR */
		if ((rtm->rtm_type == RTM_ADD) && (rtsock_find_rtm_sa(rtm, RTA_NETMASK))) {
			RLOG("MSG_PREFIXROUTE: %d", count);
			msg_array[MSG_PREFIXROUTE] = count++;
			continue;
		}

		RLOG("skipping msg type %s, try: %d", rtsock_print_cmdtype(rtm->rtm_type),
		    tries);
	}

	/* TODO: verify multicast */
	ATF_REQUIRE_MSG(count == 2, "Received only %d/2 messages", count);
	ATF_REQUIRE_MSG(msg_array[MSG_IFADDR] == 0, "ifaddr message is not the first");
}

RTM_DECLARE_ROOT_TEST(rtm_del_v4_gu_ifa_prefixroute_success,
    "Tests validness for the prefix route removal after ifaddr assignment");

ATF_TC_BODY(rtm_del_v4_gu_ifa_prefixroute_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4_iface(tc);


	ret = iface_setup_addr(c->ifname, c->addr4_str, c->plen4);

	c->rtsock_fd = rtsock_setup_socket();

	ret = iface_delete_addr(c->ifname, c->addr4_str);

	while (true) {
		rtm = rtsock_read_rtm(c->rtsock_fd, buffer, sizeof(buffer));
		/* Find RTM_ADD with netmask - this should skip both host route and LLADDR */
		if ((rtm->rtm_type == RTM_DELETE) && (rtsock_find_rtm_sa(rtm, RTA_NETMASK)))
			break;
	}

	/* This should be a message for the prefix route */
	verify_route_message(rtm, RTM_DELETE, (struct sockaddr *)&c->net4,
	    (struct sockaddr *)&c->mask4, NULL);

	/* gateway should be link sdl with ifindex of an address interface */
	verify_link_gateway(rtm, c->ifindex);

	int expected_rt_flags = RTF_DONE | RTF_PINNED;
	verify_route_message_extra(rtm, c->ifindex, expected_rt_flags);
}


ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, rtm_get_v4_exact_success);
	ATF_TP_ADD_TC(tp, rtm_get_v4_lpm_success);
	ATF_TP_ADD_TC(tp, rtm_get_v4_hostbits_success);
	ATF_TP_ADD_TC(tp, rtm_get_v4_empty_dst_failure);
	ATF_TP_ADD_TC(tp, rtm_add_v4_no_rtf_host_success);
	ATF_TP_ADD_TC(tp, rtm_add_v4_gw_direct_success);
	ATF_TP_ADD_TC(tp, rtm_del_v4_prefix_nogw_success);
	ATF_TP_ADD_TC(tp, rtm_add_v6_gu_gw_gu_direct_success);
	ATF_TP_ADD_TC(tp, rtm_del_v6_gu_prefix_nogw_success);
	ATF_TP_ADD_TC(tp, rtm_change_v4_gw_success);
	ATF_TP_ADD_TC(tp, rtm_change_v4_mtu_success);
	ATF_TP_ADD_TC(tp, rtm_change_v4_flags_success);
	ATF_TP_ADD_TC(tp, rtm_change_v6_gw_success);
	ATF_TP_ADD_TC(tp, rtm_change_v6_mtu_success);
	ATF_TP_ADD_TC(tp, rtm_change_v6_flags_success);
	/* ifaddr tests */
	ATF_TP_ADD_TC(tp, rtm_add_v6_gu_ifa_hostroute_success);
	ATF_TP_ADD_TC(tp, rtm_add_v6_gu_ifa_prefixroute_success);
	ATF_TP_ADD_TC(tp, rtm_add_v6_gu_ifa_ordered_success);
	ATF_TP_ADD_TC(tp, rtm_del_v6_gu_ifa_hostroute_success);
	ATF_TP_ADD_TC(tp, rtm_del_v6_gu_ifa_prefixroute_success);
	ATF_TP_ADD_TC(tp, rtm_add_v4_gu_ifa_ordered_success);
	ATF_TP_ADD_TC(tp, rtm_del_v4_gu_ifa_prefixroute_success);
	/* temporal routes */
	ATF_TP_ADD_TC(tp, rtm_add_v4_temporal1_success);
	ATF_TP_ADD_TC(tp, rtm_add_v6_temporal1_success);

	return (atf_no_error());
}

