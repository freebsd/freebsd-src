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

#include "util.h"

ATF_TC(basic);
ATF_TC_HEAD(basic, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(basic, dummy)
{
	char		msg[] = "test";
	ng_counter_t	r;

	ng_errors(PASS);
	ng_shutdown("hub:");
	ng_errors(FAIL);

	ng_init();
	ng_mkpeer(".", "a", "hub", "a");
	ng_name("a", "hub");
	ng_connect(".", "b", "hub:", "b");
	ng_connect(".", "c", "hub:", "c");

	/* do not bounce back */
	ng_register_data("a", get_data0);
	ng_counter_clear(r);
	ng_send_data("a", msg, sizeof(msg));
	ng_handle_events(50, r);
	ATF_CHECK(r[0] == 0);

	/* send to others */
	ng_register_data("b", get_data0);
	ng_register_data("c", get_data0);
	ng_counter_clear(r);
	ng_send_data("a", msg, sizeof(msg));
	ng_handle_events(50, r);
	ATF_CHECK(r[0] == 2);

	ng_counter_clear(r);
	ng_send_data("b", msg, sizeof(msg));
	ng_handle_events(50, r);
	ATF_CHECK(r[0] == 2);

	ng_counter_clear(r);
	ng_send_data("c", msg, sizeof(msg));
	ng_handle_events(50, r);
	ATF_CHECK(r[0] == 2);

	/* remove a link */
	ng_rmhook(".", "b");
	ng_counter_clear(r);
	ng_send_data("a", msg, sizeof(msg));
	ng_handle_events(50, r);
	ATF_CHECK(r[0] == 1);

	ng_shutdown("hub:");
}

ATF_TC(persistence);
ATF_TC_HEAD(persistence, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(persistence, dummy)
{
	ng_errors(PASS);
	ng_shutdown("hub:");
	ng_errors(FAIL);

	ng_init();
	ng_mkpeer(".", "a", "hub", "a");
	ng_name("a", "hub");

	ng_send_msg("hub:", "setpersistent");
	ng_rmhook(".", "a");

	ng_shutdown("hub:");
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
	char		msg[] = "LOOP Alert!";

#if defined(__riscv)
	if (atf_tc_get_config_var_as_bool_wd(dummy, "ci", false))
		atf_tc_skip("https://bugs.freebsd.org/259157");
#endif

	ng_errors(PASS);
	ng_shutdown("hub1:");
	ng_shutdown("hub2:");
	ng_errors(FAIL);

	ng_init();
	ng_mkpeer(".", "a", "hub", "a");
	ng_name("a", "hub1");
	ng_mkpeer(".", "b", "hub", "b");
	ng_name("b", "hub2");

	ng_register_data("a", get_data0);
	ng_register_data("b", get_data0);

	/*-
	 * Open loop
	 *
	 *    /-- hub1
	 * . <    |
	 *    \-- hub2
	 */
	ng_connect("hub1:", "xc1", "hub2:", "xc1");

	ng_counter_clear(r);
	ng_send_data("a", msg, sizeof(msg));
	ng_handle_events(50, r);
	ATF_CHECK(r[0] == 1);

	/*-
	 * Closed loop, DANGEROUS!
	 *
	 *    /-- hub1 -\
	 * . <     |    |
	 *    \-- hub2 -/
	 */
	ng_connect("hub1:", "xc2", "hub2:", "xc2");

	ng_counter_clear(r);
	ng_send_data("a", msg, sizeof(msg));
	for (i = 0; i < 10; i++)	/* don't run forever */
		if (!ng_handle_event(50, r))
			break;
	ATF_CHECK(r[0] > 7);

	ng_shutdown("hub1:");
	ng_shutdown("hub2:");
}

ATF_TC(many_hooks);
ATF_TC_HEAD(many_hooks, conf)
{
	atf_tc_set_md_var(conf, "require.user", "root");
}

ATF_TC_BODY(many_hooks, dummy)
{
	ng_counter_t	r;
	int		i;
	char		msg[] = "test";
	const int	HOOKS = 1000;

	ng_errors(PASS);
	ng_shutdown("hub:");
	ng_errors(FAIL);

	ng_init();
	ng_mkpeer(".", "a", "hub", "a");
	ng_name("a", "hub");

	ng_register_data("a", get_data0);
	ng_counter_clear(r);
	for (i = 0; i < HOOKS; i++)
	{
		char		hook[20];

		snprintf(hook, sizeof(hook), "hook%d", i);
		ng_connect(".", hook, "hub:", hook);
		ng_errors(PASS);
		ng_send_data(hook, msg, sizeof(msg));
		ng_errors(FAIL);
		if (errno != 0)
			break;
		ng_handle_events(50, r);
	}
	ATF_CHECK(r[0] > 100);
	atf_tc_expect_fail("Implementation limitation (%d)", i);
	ATF_CHECK(r[0] == HOOKS);
	atf_tc_expect_pass();

	ng_shutdown("hub:");
}


ATF_TP_ADD_TCS(hub)
{
	ATF_TP_ADD_TC(hub, basic);
	ATF_TP_ADD_TC(hub, loop);
	ATF_TP_ADD_TC(hub, persistence);
	ATF_TP_ADD_TC(hub, many_hooks);

	return atf_no_error();
}
