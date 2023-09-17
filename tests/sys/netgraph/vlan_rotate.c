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
#include <stdlib.h>
#include <stdio.h>

#include <net/ethernet.h>
#include <netinet/in.h>

#include "util.h"
#include <netgraph/ng_bridge.h>

struct vlan
{
	uint16_t	proto;
	uint16_t	tag;
}		__packed;

struct frame
{
	u_char		dst[ETHER_ADDR_LEN];
	u_char		src[ETHER_ADDR_LEN];
	struct vlan	vlan[10];
}		__packed;

static struct frame msg = {
	.src = {2, 4, 6, 1, 3, 5},
	.dst = {2, 4, 6, 1, 3, 7},
	.vlan[0] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(1, 0, 0))},
	.vlan[1] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(2, 0, 0))},
	.vlan[2] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(3, 0, 0))},
	.vlan[3] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(4, 0, 0))},
	.vlan[4] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(5, 0, 0))},
	.vlan[5] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(6, 0, 0))},
	.vlan[6] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(7, 0, 0))},
	.vlan[7] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(8, 0, 0))},
	.vlan[8] = {htons(ETHERTYPE_VLAN), htons(EVL_MAKETAG(9, 0, 0))},
	.vlan[9] = {0}
};

static void	_basic(int);
static void	get_vlan(void *data, size_t len, void *ctx);

static void
get_vlan(void *data, size_t len, void *ctx)
{
	int	       *v = ctx, i;
	struct frame   *f = data;

	(void)len;
	for (i = 0; i < 10; i++)
		v[i] = EVL_VLANOFTAG(ntohs(f->vlan[i].tag));
}

static void
_basic(int direction)
{
	int		r[10];
	int		i, rot, len;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("vr:");
	ng_errors(FAIL);

	ng_mkpeer(".", "a", "vlan_rotate", direction > 0 ? "original" : "ordered");
	ng_name("a", "vr");
	ng_connect(".", "b", "vr:", direction > 0 ? "ordered" : "original");
	ng_register_data("b", get_vlan);

	for (len = 9; len > 0; len--)
	{
		/* reduce the number of vlans */
		msg.vlan[len].proto = htons(ETHERTYPE_IP);

		for (rot = -len + 1; rot < len; rot++)
		{
			char		cmd[40];

			/* set rotation offset */
			snprintf(cmd, sizeof(cmd), "setconf { min=0 max=9 rot=%d }", rot);
			ng_send_msg("vr:", cmd);

			ng_send_data("a", &msg, sizeof(msg));
			ng_handle_events(50, &r);

			/* check rotation */
			for (i = 0; i < len; i++)
			{
				int		expect = (2 * len + i - direction * rot) % len + 1;
				int		vlan = r[i];

				ATF_CHECK_MSG(vlan == expect,
				 "len=%d rot=%d i=%d -> vlan=%d, expect=%d",
					      len, rot, i, r[i], expect);
			}
		}
	}

	ng_shutdown("vr:");
}

ATF_TC(basic);
ATF_TC_HEAD(basic, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(basic, dummy)
{
	_basic(1);
}

ATF_TC(reverse);
ATF_TC_HEAD(reverse, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(reverse, dummy)
{
	_basic(-1);
}

static void	_ethertype(int);
static void	get_ethertype(void *data, size_t len, void *ctx);

static void
get_ethertype(void *data, size_t len, void *ctx)
{
	int	       *v = ctx, i;
	struct frame   *f = data;

	(void)len;
	for (i = 0; i < 10; i++)
		v[i] = ntohs(f->vlan[i].proto);
}

static void
_ethertype(int direction)
{
	int		r[10];
	int		i, rounds = 20;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("vr:");
	ng_errors(FAIL);

	ng_mkpeer(".", "a", "vlan_rotate", direction > 0 ? "original" : "ordered");
	ng_name("a", "vr");
	ng_connect(".", "b", "vr:", direction > 0 ? "ordered" : "original");
	ng_register_data("b", get_ethertype);

	while (rounds-- > 0)
	{
		char		cmd[40];
		int		len = 9;
		int		rot = rand() % (2 * len - 1) - len + 1;
		int		vlan[10];

		for (i = 0; i < len; i++)
		{
			switch (rand() % 3)
			{
			default:
				msg.vlan[i].proto = htons(ETHERTYPE_VLAN);
				break;
			case 1:
				msg.vlan[i].proto = htons(ETHERTYPE_QINQ);
				break;
			case 2:
				msg.vlan[i].proto = htons(ETHERTYPE_8021Q9100);
				break;
			}
		}
		msg.vlan[i].proto = htons(ETHERTYPE_IP);

		for (i = 0; i < len; i++)
			vlan[i] = msg.vlan[i].proto;

		snprintf(cmd, sizeof(cmd), "setconf { min=0 max=9 rot=%d }", rot);
		ng_send_msg("vr:", cmd);

		bzero(r, sizeof(r));
		ng_send_data("a", &msg, sizeof(msg));
		ng_handle_events(50, &r);

		/* check rotation */
		for (i = 0; i < len; i++)
		{
			int		expect = (2 * len + i - direction * rot) % len;

			ATF_CHECK_MSG(r[i] == ntohs(vlan[expect]),
			 "len=%d rot=%d i=%d -> vlan=%04x, expect(%d)=%04x",
			    len, rot, i, ntohs(r[i]), expect, vlan[expect]);
		}
	}

	ng_shutdown("vr:");
}

ATF_TC(ethertype);
ATF_TC_HEAD(ethertype, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(ethertype, dummy)
{
	_ethertype(1);
}

ATF_TC(typeether);
ATF_TC_HEAD(typeether, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(typeether, dummy)
{
	_ethertype(-1);
}

ATF_TC(minmax);
ATF_TC_HEAD(minmax, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(minmax, dummy)
{
	ng_counter_t	r;
	int		len;

	ng_init();
	ng_errors(PASS);
	ng_shutdown("vr:");
	ng_errors(FAIL);

	ng_mkpeer(".", "a", "vlan_rotate", "original");
	ng_name("a", "vr");
	ng_connect(".", "b", "vr:", "ordered");
	ng_connect(".", "c", "vr:", "excessive");
	ng_connect(".", "d", "vr:", "incomplete");
	ng_register_data("a", get_data0);
	ng_register_data("b", get_data1);
	ng_register_data("c", get_data2);
	ng_register_data("d", get_data3);

	ng_send_msg("vr:", "setconf { min=3 max=7 rot=0 }");
	for (len = 9; len > 0; len--)
	{
		/* reduce the number of vlans */
		msg.vlan[len].proto = htons(ETHERTYPE_IP);

		ng_counter_clear(r);
		ng_send_data("a", &msg, sizeof(msg));
		ng_handle_events(50, &r);
		if (len < 3)
			ATF_CHECK(r[0] == 0 && r[1] == 0 && r[2] == 0 && r[3] == 1);
		else if (len > 7)
			ATF_CHECK(r[0] == 0 && r[1] == 0 && r[2] == 1 && r[3] == 0);
		else
			ATF_CHECK(r[0] == 0 && r[1] == 1 && r[2] == 0 && r[3] == 0);

		ng_counter_clear(r);
		ng_send_data("b", &msg, sizeof(msg));
		ng_handle_events(50, &r);
		if (len < 3)
			ATF_CHECK(r[0] == 0 && r[1] == 0 && r[2] == 0 && r[3] == 1);
		else if (len > 7)
			ATF_CHECK(r[0] == 0 && r[1] == 0 && r[2] == 1 && r[3] == 0);
		else
			ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 0 && r[3] == 0);

		ng_counter_clear(r);
		ng_send_data("c", &msg, sizeof(msg));
		ng_handle_events(50, &r);
		ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 0 && r[3] == 0);

		ng_counter_clear(r);
		ng_send_data("d", &msg, sizeof(msg));
		ng_handle_events(50, &r);
		ATF_CHECK(r[0] == 1 && r[1] == 0 && r[2] == 0 && r[3] == 0);
	}

	ng_shutdown("vr:");
}

ATF_TP_ADD_TCS(vlan_rotate)
{
	/* Use "dd if=/dev/random bs=2 count=1 | od -x" to reproduce */
	srand(0xb93b);

	ATF_TP_ADD_TC(vlan_rotate, basic);
	ATF_TP_ADD_TC(vlan_rotate, ethertype);
	ATF_TP_ADD_TC(vlan_rotate, reverse);
	ATF_TP_ADD_TC(vlan_rotate, typeether);
	ATF_TP_ADD_TC(vlan_rotate, minmax);

	return atf_no_error();
}
