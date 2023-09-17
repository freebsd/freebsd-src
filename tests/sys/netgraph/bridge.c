/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 Lutz Donnerhacke
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <atf-c.h>
#include <errno.h>
#include <stdio.h>

#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include "util.h"
#include <netgraph/ng_bridge.h>

static void	get_tablesize(char const *source, struct ng_mesg *msg, void *ctx);
struct gettable
{
	u_int32_t	tok;
	int		cnt;
};

struct frame4
{
	struct ether_header eh;
	struct ip	ip;
	char		data[64];
};
struct frame6
{
	struct ether_header eh;
	struct ip6_hdr	ip;
	char		data[64];
};

static struct frame4 msg4 = {
	.ip.ip_v = 4,
	.ip.ip_hl = 5,
	.ip.ip_ttl = 1,
	.ip.ip_p = 254,
	.ip.ip_src = {htonl(0x0a00dead)},
	.ip.ip_dst = {htonl(0x0a00beef)},
	.ip.ip_len = 32,
	.eh.ether_type = ETHERTYPE_IP,
	.eh.ether_shost = {2, 4, 6},
	.eh.ether_dhost = {2, 4, 6},
};


ATF_TC(basic);
ATF_TC_HEAD(basic, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(basic, dummy)
{
	ng_counter_t	r;
	struct gettable	rm;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("bridge:");
	ng_errors(FAIL);

	ng_mkpeer(".", "a", "bridge", "link0");
	ng_name("a", "bridge");
	ng_connect(".", "b", "bridge:", "link1");
	ng_connect(".", "c", "bridge:", "link2");

	/* do not bounce back */
	ng_register_data("a", get_data0);
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	ng_send_data("a", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0);

	/* send to others */
	ng_register_data("b", get_data1);
	ng_register_data("c", get_data2);
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	ng_send_data("a", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1);

	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 2;
	ng_send_data("b", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 1);

	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 3;
	ng_send_data("c", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 1 && r[2] == 0);

	/* send to learned unicast */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	msg4.eh.ether_dhost[5] = 3;
	ng_send_data("a", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 0 && r[2] == 1);

	/* inspect mac table */
	ng_register_msg(get_tablesize);
	rm.tok = ng_send_msg("bridge:", "gettable");
	rm.cnt = 0;
	ng_handle_events(50, &rm);
	ATF_CHECK(rm.cnt == 3);

	/* remove a link */
	ng_rmhook(".", "b");
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	msg4.eh.ether_dhost[5] = 0;
	ng_send_data("a", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 0 && r[2] == 1);

	/* inspect mac table */
	ng_register_msg(get_tablesize);
	rm.tok = ng_send_msg("bridge:", "gettable");
	rm.cnt = 0;
	ng_handle_events(50, &rm);
	ATF_CHECK(rm.cnt == 2);

	ng_shutdown("bridge:");
}

ATF_TC(persistence);
ATF_TC_HEAD(persistence, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(persistence, dummy)
{
	ng_init();
	ng_errors(PASS);
	ng_shutdown("bridge:");
	ng_errors(FAIL);

	ng_mkpeer(".", "a", "bridge", "link0");
	ng_name("a", "bridge");

	ng_send_msg("bridge:", "setpersistent");
	ng_rmhook(".", "a");

	ng_shutdown("bridge:");
}

ATF_TC(loop);
ATF_TC_HEAD(loop, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(loop, dummy)
{
	ng_counter_t	r;
	int		i;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("bridge1:");
	ng_shutdown("bridge2:");
	ng_errors(FAIL);

	ng_mkpeer(".", "a", "bridge", "link0");
	ng_name("a", "bridge1");
	ng_mkpeer(".", "b", "bridge", "link1");
	ng_name("b", "bridge2");

	ng_register_data("a", get_data0);
	ng_register_data("b", get_data1);

	/*-
	 * Open loop
	 *
	 *    /-- bridge1
	 * . <    |
	 *    \-- bridge2
	 */
	ng_connect("bridge1:", "link11", "bridge2:", "link11");

	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	ng_send_data("a", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1);

	/*-
	 * Closed loop, DANGEROUS!
	 *
	 *    /-- bridge1 -\
	 * . <     |       |
	 *    \-- bridge2 -/
	 */
	ng_connect("bridge1:", "link12", "bridge2:", "link12");

	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	ng_errors(PASS);
	ng_send_data("a", &msg4, sizeof(msg4));
	ATF_CHECK_ERRNO(ELOOP, errno != 0);	/* loop might be detected */
	ng_errors(FAIL);
	for (i = 0; i < 10; i++)	/* don't run forever */
		if (!ng_handle_event(50, &r))
			break;
	ATF_CHECK(r[0] == 0 && r[1] == 1);

	ng_shutdown("bridge1:");
	ng_shutdown("bridge2:");
}

ATF_TC(many_unicasts);
ATF_TC_HEAD(many_unicasts, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(many_unicasts, dummy)
{
	ng_counter_t	r;
	int		i;
	const int	HOOKS = 1000;
	struct gettable	rm;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("bridge:");
	ng_errors(FAIL);

	ng_mkpeer(".", "a", "bridge", "link0");
	ng_name("a", "bridge");
	ng_register_data("a", get_data0);

	/* learn MAC */
	ng_counter_clear(r);
	msg4.eh.ether_shost[3] = 0xff;
	ng_send_data("a", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0);

	/* use learned MAC as destination */
	msg4.eh.ether_shost[3] = 0;
	msg4.eh.ether_dhost[3] = 0xff;

	/* now send */
	ng_counter_clear(r);
	for (i = 1; i <= HOOKS; i++)
	{
		char		hook[20];

		snprintf(hook, sizeof(hook), "link%d", i);
		ng_connect(".", hook, "bridge:", hook);
		ng_register_data(hook, get_data2);

		msg4.eh.ether_shost[4] = i >> 8;
		msg4.eh.ether_shost[5] = i & 0xff;
		ng_errors(PASS);
		ng_send_data(hook, &msg4, sizeof(msg4));
		ng_errors(FAIL);
		if (errno != 0)
			break;
		ng_handle_events(50, &r);
	}
	ATF_CHECK(r[0] == HOOKS && r[2] == 0);

	/* inspect mac table */
	ng_register_msg(get_tablesize);
	rm.cnt = 0;
	ng_errors(PASS);
	rm.tok = ng_send_msg("bridge:", "gettable");
	ng_errors(FAIL);
	if (rm.tok == (u_int32_t)-1)
	{
		ATF_CHECK_ERRNO(ENOBUFS, 1);
		atf_tc_expect_fail("response too large");
	}
	ng_handle_events(50, &rm);
	ATF_CHECK(rm.cnt == HOOKS + 1);
	atf_tc_expect_pass();

	ng_shutdown("bridge:");
}

ATF_TC(many_broadcasts);
ATF_TC_HEAD(many_broadcasts, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(many_broadcasts, dummy)
{
	ng_counter_t	r;
	int		i;
	const int	HOOKS = 1000;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("bridge:");
	ng_errors(FAIL);

	ng_mkpeer(".", "a", "bridge", "link0");
	ng_name("a", "bridge");
	ng_register_data("a", get_data0);

	/* learn MAC */
	ng_counter_clear(r);
	msg4.eh.ether_shost[3] = 0xff;
	ng_send_data("a", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0);

	/* use broadcast MAC */
	msg4.eh.ether_shost[3] = 0;
	memset(msg4.eh.ether_dhost, 0xff, sizeof(msg4.eh.ether_dhost));

	/* now send */
	ng_counter_clear(r);
	for (i = 1; i <= HOOKS; i++)
	{
		char		hook[20];

		snprintf(hook, sizeof(hook), "link%d", i);
		ng_connect(".", hook, "bridge:", hook);
		ng_register_data(hook, get_data3);

		msg4.eh.ether_shost[4] = i >> 8;
		msg4.eh.ether_shost[5] = i & 0xff;
		ng_errors(PASS);
		ng_send_data(hook, &msg4, sizeof(msg4));
		ng_errors(FAIL);
		if (errno != 0)
			break;
		ng_handle_events(50, &r);
	}
	ATF_CHECK(r[0] > 100 && r[3] > 100);
	if (i < HOOKS)
		atf_tc_expect_fail("netgraph queue full (%d)", i);
	ATF_CHECK(r[0] == HOOKS);
	atf_tc_expect_pass();

	ng_shutdown("bridge:");
}

ATF_TC(uplink_private);
ATF_TC_HEAD(uplink_private, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(uplink_private, dummy)
{
	ng_counter_t	r;
	struct gettable	rm;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("bridge:");

	ng_mkpeer(".", "u1", "bridge", "uplink1");
	if (errno > 0)
		atf_tc_skip("uplinks are not supported.");
	ng_errors(FAIL);
	ng_name("u1", "bridge");
	ng_register_data("u1", get_data1);
	ng_connect(".", "u2", "bridge:", "uplink2");
	ng_register_data("u2", get_data2);
	ng_connect(".", "l0", "bridge:", "link0");
	ng_register_data("l0", get_data0);
	ng_connect(".", "l3", "bridge:", "link3");
	ng_register_data("l3", get_data3);

	/* unknown unicast 0 from uplink1 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	ng_send_data("u1", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 0 && r[2] == 1 && r[3] == 0);

	/* unknown unicast 2 from link0 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 0;
	msg4.eh.ether_dhost[5] = 2;
	ng_send_data("l0", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1 && r[3] == 0);

	/* known unicast 0 from uplink2 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 2;
	msg4.eh.ether_dhost[5] = 0;
	ng_send_data("u2", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 0 && r[3] == 0);

	/* known unicast 0 from link3 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 3;
	msg4.eh.ether_dhost[5] = 0;
	ng_send_data("l3", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 0 && r[3] == 0);

	/* (un)known unicast 2 from uplink1 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	msg4.eh.ether_dhost[5] = 2;
	ng_send_data("u1", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 0 && r[2] == 1 && r[3] == 0);

	/* (un)known unicast 2 from link0 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 0;
	ng_send_data("l0", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1 && r[3] == 0);

	/* unknown multicast 2 from uplink1 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	msg4.eh.ether_dhost[0] = 0xff;
	ng_send_data("u1", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 1 && r[3] == 1);

	/* unknown multicast 2 from link0 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 0;
	ng_send_data("l0", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1 && r[3] == 1);

	/* broadcast from uplink1 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	memset(msg4.eh.ether_dhost, 0xff, sizeof(msg4.eh.ether_dhost));
	ng_send_data("u1", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 1 && r[3] == 1);

	/* broadcast from link0 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 0;
	ng_send_data("l0", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1 && r[3] == 1);

	/* inspect mac table */
	ng_register_msg(get_tablesize);
	rm.tok = ng_send_msg("bridge:", "gettable");
	rm.cnt = 0;
	ng_handle_events(50, &rm);
	ATF_CHECK(rm.cnt == 2);

	ng_shutdown("bridge:");
}

ATF_TC(uplink_classic);
ATF_TC_HEAD(uplink_classic, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(uplink_classic, dummy)
{
	ng_counter_t	r;
	struct gettable	rm;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("bridge:");

	ng_mkpeer(".", "l0", "bridge", "link0");
	if (errno > 0)
		atf_tc_skip("uplinks are not supported.");
	ng_errors(FAIL);
	ng_name("l0", "bridge");
	ng_register_data("l0", get_data0);
	ng_connect(".", "u1", "bridge:", "uplink1");
	ng_register_data("u1", get_data1);
	ng_connect(".", "u2", "bridge:", "uplink2");
	ng_register_data("u2", get_data2);
	ng_connect(".", "l3", "bridge:", "link3");
	ng_register_data("l3", get_data3);

	/* unknown unicast 0 from uplink1 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	ng_send_data("u1", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 1 && r[3] == 1);

	/* unknown unicast 2 from link0 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 0;
	msg4.eh.ether_dhost[5] = 2;
	ng_send_data("l0", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1 && r[3] == 1);

	/* known unicast 0 from uplink2 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 2;
	msg4.eh.ether_dhost[5] = 0;
	ng_send_data("u2", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 0 && r[3] == 0);

	/* known unicast 0 from link3 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 3;
	msg4.eh.ether_dhost[5] = 0;
	ng_send_data("l3", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 0 && r[3] == 0);

	/* (un)known unicast 2 from uplink1 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	msg4.eh.ether_dhost[5] = 2;
	ng_send_data("u1", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 1 && r[3] == 1);

	/* (un)known unicast 2 from link0 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 0;
	ng_send_data("l0", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1 && r[3] == 1);

	/* unknown multicast 2 from uplink1 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	msg4.eh.ether_dhost[0] = 0xff;
	ng_send_data("u1", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 1 && r[3] == 1);

	/* unknown multicast 2 from link0 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 0;
	ng_send_data("l0", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1 && r[3] == 1);

	/* broadcast from uplink1 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 1;
	memset(msg4.eh.ether_dhost, 0xff, sizeof(msg4.eh.ether_dhost));
	ng_send_data("u1", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 1 && r[3] == 1);

	/* broadcast from link0 */
	ng_counter_clear(r);
	msg4.eh.ether_shost[5] = 0;
	ng_send_data("l0", &msg4, sizeof(msg4));
	ng_handle_events(50, &r);
	ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 1 && r[3] == 1);

	/* inspect mac table */
	ng_register_msg(get_tablesize);
	rm.tok = ng_send_msg("bridge:", "gettable");
	rm.cnt = 0;
	ng_handle_events(50, &rm);
	ATF_CHECK(rm.cnt == 2);

	ng_shutdown("bridge:");
}

ATF_TP_ADD_TCS(bridge)
{
	ATF_TP_ADD_TC(bridge, basic);
	ATF_TP_ADD_TC(bridge, loop);
	ATF_TP_ADD_TC(bridge, persistence);
	ATF_TP_ADD_TC(bridge, many_unicasts);
	ATF_TP_ADD_TC(bridge, many_broadcasts);
	ATF_TP_ADD_TC(bridge, uplink_private);
	ATF_TP_ADD_TC(bridge, uplink_classic);

	return atf_no_error();
}

static void
get_tablesize(char const *source, struct ng_mesg *msg, void *ctx)
{
	struct gettable *rm = ctx;
	struct ng_bridge_host_ary *gt = (void *)msg->data;

	fprintf(stderr, "Response from %s to query %d\n", source, msg->header.token);
	if (rm->tok == msg->header.token)
		rm->cnt = gt->numHosts;
}
