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
 */

#include "rtsock_common.h"
#include "rtsock_config.h"

static void
jump_vnet(struct rtsock_test_config *c, const atf_tc_t *tc)
{
	char vnet_name[512];

	snprintf(vnet_name, sizeof(vnet_name), "vt-%s", atf_tc_get_ident(tc));
	RLOG("jumping to %s", vnet_name);

	vnet_switch_one(vnet_name, c->ifname);

	/* Update ifindex cache */
	c->ifindex = if_nametoindex(c->ifname);
}

static inline struct rtsock_test_config *
presetup_ipv6(const atf_tc_t *tc)
{
	struct rtsock_test_config *c;
	int ret;

	c = config_setup(tc, NULL);

	jump_vnet(c, tc);

	ret = iface_turn_up(c->ifname);
	ATF_REQUIRE_MSG(ret == 0, "Unable to turn up %s", c->ifname);
	ret = iface_enable_ipv6(c->ifname);
	ATF_REQUIRE_MSG(ret == 0, "Unable to enable IPv6 on %s", c->ifname);

	c->rtsock_fd = rtsock_setup_socket();

	return (c);
}

static inline struct rtsock_test_config *
presetup_ipv4(const atf_tc_t *tc)
{
	struct rtsock_test_config *c;
	int ret;

	c = config_setup(tc, NULL);

	jump_vnet(c, tc);

	/* assumes ifconfig doing IFF_UP */
	ret = iface_setup_addr(c->ifname, c->addr4_str, c->plen4);
	ATF_REQUIRE_MSG(ret == 0, "ifconfig failed");

	c->rtsock_fd = rtsock_setup_socket();

	return (c);
}

static void
prepare_route_message(struct rt_msghdr *rtm, int cmd, struct sockaddr *dst,
  struct sockaddr *gw)
{

	rtsock_prepare_route_message(rtm, cmd, dst, NULL, gw);

	rtm->rtm_flags |= (RTF_HOST | RTF_STATIC | RTF_LLDATA);
}

/* TESTS */
#define	DECLARE_TEST_VARS					\
	char buffer[2048], msg[512];				\
	ssize_t len;						\
	int ret;						\
	struct rtsock_test_config *c;				\
	struct rt_msghdr *rtm = (struct rt_msghdr *)buffer;	\
	struct sockaddr *sa;					\
								\

#define	DECLARE_CLEANUP_VARS					\
	struct rtsock_test_config *c = config_setup(tc);	\
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

RTM_DECLARE_ROOT_TEST(rtm_add_v6_ll_lle_success, "Tests addition of link-local IPv6 ND entry");
ATF_TC_BODY(rtm_add_v6_ll_lle_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6(tc);

	char str_buf[128];
	struct sockaddr_in6 sin6;
	/* Interface here is optional. XXX: verify kernel side. */
	char *v6addr = "fe80::4242:4242";
	snprintf(str_buf, sizeof(str_buf), "%s%%%s", v6addr, c->ifname);
	sa_convert_str_to_sa(str_buf, (struct sockaddr *)&sin6);

	struct sockaddr_dl ether;
	snprintf(str_buf, sizeof(str_buf), "%s%%%s", c->remote_lladdr, c->ifname);
	sa_convert_str_to_sa(str_buf, (struct sockaddr *)&ether);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&sin6, (struct sockaddr *)&ether);
	rtsock_send_rtm(c->rtsock_fd, rtm);

	/*
	 * Got message of size 240 on 2019-12-17 15:06:51
	 * RTM_ADD: Add Route: len 240, pid: 0, seq 0, errno 0, flags: <UP,HOST,DONE,LLINFO>
	 * sockaddrs: 0x3 <DST,GATEWAY>
	 *  af=inet6 len=28 addr=fe80::4242:4242 scope_id=3 if_name=tap4242
	 *  af=link len=54 sdl_index=3 if_name=tap4242 addr=52:54:00:14:E3:10
	 */

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	sa = rtsock_find_rtm_sa(rtm, RTA_DST);
	ret = sa_equal_msg(sa, (struct sockaddr *)&sin6, msg, sizeof(msg));
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "DST sa diff: %s", msg);

	sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
	int sa_flags = SA_F_IGNORE_IFNAME | SA_F_IGNORE_IFTYPE | SA_F_IGNORE_MEMCMP;
	ret = sa_equal_msg_flags(sa, (struct sockaddr *)&ether, msg, sizeof(msg), sa_flags);
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "GATEWAY sa diff: %s", msg);

#if 0
	/* Disable the check until https://reviews.freebsd.org/D22003 merge */
	/* Some additional checks to verify kernel has filled in interface data */
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;
	RTSOCK_ATF_REQUIRE_MSG(rtm, sdl->sdl_type > 0, "sdl_type not set");
#endif
}

RTM_DECLARE_ROOT_TEST(rtm_add_v6_gu_lle_success, "Tests addition of global IPv6 ND entry");
ATF_TC_BODY(rtm_add_v6_gu_lle_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6(tc);

	char str_buf[128];

	struct sockaddr_in6 sin6;
	sin6 = c->net6;
#define _s6_addr32 __u6_addr.__u6_addr32
	sin6.sin6_addr._s6_addr32[3] = htonl(0x42424242);
#undef _s6_addr32

	struct sockaddr_dl ether;
	snprintf(str_buf, sizeof(str_buf), "%s%%%s", c->remote_lladdr, c->ifname);
	sa_convert_str_to_sa(str_buf, (struct sockaddr *)&ether);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&sin6, (struct sockaddr *)&ether);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	/*
	 * Got message of size 240 on 2019-12-17 14:56:43
	 * RTM_ADD: Add Route: len 240, pid: 0, seq 0, errno 0, flags: <UP,HOST,DONE,LLINFO>
	 * sockaddrs: 0x3 <DST,GATEWAY>
	 *  af=inet6 len=28 addr=2001:db8::4242:4242
 	 *  af=link len=54 sdl_index=3 if_name=tap4242 addr=52:54:00:14:E3:10
	 */

	/* XXX: where is uRPF?! this should fail */

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	sa = rtsock_find_rtm_sa(rtm, RTA_DST);
	ret = sa_equal_msg(sa, (struct sockaddr *)&sin6, msg, sizeof(msg));
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "DST sa diff: %s", msg);

	sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
	int sa_flags = SA_F_IGNORE_IFNAME | SA_F_IGNORE_IFTYPE | SA_F_IGNORE_MEMCMP;
	ret = sa_equal_msg_flags(sa, (struct sockaddr *)&ether, msg, sizeof(msg), sa_flags);
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "GATEWAY sa diff: %s", msg);

#if 0
	/* Disable the check until https://reviews.freebsd.org/D22003 merge */
	/* Some additional checks to verify kernel has filled in interface data */
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;
	RTSOCK_ATF_REQUIRE_MSG(rtm, sdl->sdl_type > 0, "sdl_type not set");
#endif
}

RTM_DECLARE_ROOT_TEST(rtm_add_v4_gu_lle_success, "Tests addition of IPv4 ARP entry");
ATF_TC_BODY(rtm_add_v4_gu_lle_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	char str_buf[128];

	struct sockaddr_in sin;
	sin = c->addr4;
	/* Use the next IPv4 address after self */
	sin.sin_addr.s_addr = htonl(ntohl(sin.sin_addr.s_addr) + 1);

	struct sockaddr_dl ether;
	snprintf(str_buf, sizeof(str_buf), "%s%%%s", c->remote_lladdr, c->ifname);
	sa_convert_str_to_sa(str_buf, (struct sockaddr *)&ether);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&sin, (struct sockaddr *)&ether);

	len = rtsock_send_rtm(c->rtsock_fd, rtm);

	/*
	 * RTM_ADD: Add Route: len 224, pid: 43131, seq 42, errno 0, flags: <HOST,DONE,LLINFO,STATIC>
	 * sockaddrs: 0x3 <DST,GATEWAY>
	 *  af=inet len=16 addr=192.0.2.2
	 *  af=link len=54 sdl_index=3 if_name=tap4242 addr=52:54:00:14:E3:10
	 */

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	sa = rtsock_find_rtm_sa(rtm, RTA_DST);
	ret = sa_equal_msg(sa, (struct sockaddr *)&sin, msg, sizeof(msg));
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "DST sa diff: %s", msg);

	sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
	int sa_flags = SA_F_IGNORE_IFNAME | SA_F_IGNORE_IFTYPE | SA_F_IGNORE_MEMCMP;
	ret = sa_equal_msg_flags(sa, (struct sockaddr *)&ether, msg, sizeof(msg), sa_flags);
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "GATEWAY sa diff: %s", msg);

	/*
	 * TODO: Currently kernel code does not set sdl_type, contrary to IPv6.
	 */
}

RTM_DECLARE_ROOT_TEST(rtm_del_v6_ll_lle_success, "Tests removal of link-local IPv6 ND entry");
ATF_TC_BODY(rtm_del_v6_ll_lle_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6(tc);

	char str_buf[128];

	struct sockaddr_in6 sin6;
	/* Interface here is optional. XXX: verify kernel side. */
	char *v6addr = "fe80::4242:4242";
	snprintf(str_buf, sizeof(str_buf), "%s%%%s", v6addr, c->ifname);
	sa_convert_str_to_sa(str_buf, (struct sockaddr *)&sin6);

	struct sockaddr_dl ether;
	snprintf(str_buf, sizeof(str_buf), "%s%%%s", c->remote_lladdr, c->ifname);
	sa_convert_str_to_sa(str_buf, (struct sockaddr *)&ether);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&sin6, (struct sockaddr *)&ether);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	/* Successfully added an entry, let's try to remove it. */
	prepare_route_message(rtm, RTM_DELETE, (struct sockaddr *)&sin6, (struct sockaddr *)&ether);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_type == RTM_DELETE, "rtm_type is not delete");

	sa = rtsock_find_rtm_sa(rtm, RTA_DST);
	ret = sa_equal_msg(sa, (struct sockaddr *)&sin6, msg, sizeof(msg));
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "DST sa diff: %s", msg);

	sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
	int sa_flags = SA_F_IGNORE_IFNAME | SA_F_IGNORE_IFTYPE | SA_F_IGNORE_MEMCMP;
	ret = sa_equal_msg_flags(sa, (struct sockaddr *)&ether, msg, sizeof(msg), sa_flags);
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "GATEWAY sa diff: %s", msg);

	/*
	 * TODO: Currently kernel code does not set sdl_type on delete.
	 */
}

RTM_DECLARE_ROOT_TEST(rtm_del_v6_gu_lle_success, "Tests removal of global IPv6 ND entry");
ATF_TC_BODY(rtm_del_v6_gu_lle_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv6(tc);

	char str_buf[128];

	struct sockaddr_in6 sin6;
	sin6 = c->net6;
#define _s6_addr32 __u6_addr.__u6_addr32
	sin6.sin6_addr._s6_addr32[3] = htonl(0x42424242);
#undef _s6_addr32

	struct sockaddr_dl ether;
	snprintf(str_buf, sizeof(str_buf), "%s%%%s", c->remote_lladdr, c->ifname);
	sa_convert_str_to_sa(str_buf, (struct sockaddr *)&ether);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&sin6, (struct sockaddr *)&ether);

	len = rtsock_send_rtm(c->rtsock_fd, rtm);

	/* Successfully added an entry, let's try to remove it. */
	prepare_route_message(rtm, RTM_DELETE, (struct sockaddr *)&sin6, (struct sockaddr *)&ether);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_type == RTM_DELETE, "rtm_type is not delete");

	sa = rtsock_find_rtm_sa(rtm, RTA_DST);
	ret = sa_equal_msg(sa, (struct sockaddr *)&sin6, msg, sizeof(msg));
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "DST sa diff: %s", msg);

	sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
	int sa_flags = SA_F_IGNORE_IFNAME | SA_F_IGNORE_IFTYPE | SA_F_IGNORE_MEMCMP;
	ret = sa_equal_msg_flags(sa, (struct sockaddr *)&ether, msg, sizeof(msg), sa_flags);
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "GATEWAY sa diff: %s", msg);

	/*
	 * TODO: Currently kernel code does not set sdl_type on delete.
	 */
}

RTM_DECLARE_ROOT_TEST(rtm_del_v4_gu_lle_success, "Tests removal of IPv4 ARP entry");
ATF_TC_BODY(rtm_del_v4_gu_lle_success, tc)
{
	DECLARE_TEST_VARS;

	c = presetup_ipv4(tc);

	char str_buf[128];

	struct sockaddr_in sin;
	sin = c->addr4;
	/* Use the next IPv4 address after self */
	sin.sin_addr.s_addr = htonl(ntohl(sin.sin_addr.s_addr) + 1);

	struct sockaddr_dl ether;
	snprintf(str_buf, sizeof(str_buf), "%s%%%s", c->remote_lladdr, c->ifname);
	sa_convert_str_to_sa(str_buf, (struct sockaddr *)&ether);

	prepare_route_message(rtm, RTM_ADD, (struct sockaddr *)&sin, (struct sockaddr *)&ether);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	/* We successfully added an entry, let's try to remove it. */
	prepare_route_message(rtm, RTM_DELETE, (struct sockaddr *)&sin, (struct sockaddr *)&ether);

	rtsock_send_rtm(c->rtsock_fd, rtm);

	rtm = rtsock_read_rtm_reply(c->rtsock_fd, buffer, sizeof(buffer), rtm->rtm_seq);

	RTSOCK_ATF_REQUIRE_MSG(rtm, rtm->rtm_type == RTM_DELETE, "rtm_type is not delete");

	sa = rtsock_find_rtm_sa(rtm, RTA_DST);
	ret = sa_equal_msg(sa, (struct sockaddr *)&sin, msg, sizeof(msg));
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "DST sa diff: %s", msg);

	sa = rtsock_find_rtm_sa(rtm, RTA_GATEWAY);
	int sa_flags = SA_F_IGNORE_IFNAME | SA_F_IGNORE_IFTYPE | SA_F_IGNORE_MEMCMP;
	ret = sa_equal_msg_flags(sa, (struct sockaddr *)&ether, msg, sizeof(msg), sa_flags);
	RTSOCK_ATF_REQUIRE_MSG(rtm, ret != 0, "GATEWAY sa diff: %s", msg);

	/*
	 * TODO: Currently kernel code does not set sdl_type, contrary to IPv6.
	 */
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, rtm_add_v6_ll_lle_success);
	ATF_TP_ADD_TC(tp, rtm_add_v6_gu_lle_success);
	ATF_TP_ADD_TC(tp, rtm_add_v4_gu_lle_success);
	ATF_TP_ADD_TC(tp, rtm_del_v6_ll_lle_success);
	ATF_TP_ADD_TC(tp, rtm_del_v6_gu_lle_success);
	ATF_TP_ADD_TC(tp, rtm_del_v4_gu_lle_success);

	return (atf_no_error());
}


