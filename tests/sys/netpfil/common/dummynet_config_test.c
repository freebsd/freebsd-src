/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Osvaldo Janeri Filho <ojanerif@amd.com>
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
 * ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * do_config() validates that each command's declared length fits in the
 * supplied buffer, but the DN_LINK, DN_PROFILE, DN_SCH and DN_FS cases
 * copy a fixed sizeof(struct) that is larger than struct dn_id.  A command
 * whose oid.len is only sizeof(struct dn_id) must be rejected rather than
 * read beyond the buffer.
 */
static void
check_short_object(uint8_t type)
{
	struct dn_id o;
	int s, rv;

	s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	ATF_REQUIRE_MSG(s >= 0, "raw socket: %s", strerror(errno));

	memset(&o, 0, sizeof(o));
	o.len = sizeof(o);
	o.type = type;
	o.id = DN_API_VERSION;

	rv = setsockopt(s, IPPROTO_IP, IP_DUMMYNET3, &o, sizeof(o));
	ATF_CHECK_MSG(rv == -1 && errno == EINVAL,
	    "type %u: expected EINVAL, got rv=%d errno=%d", type, rv, errno);

	close(s);
}

static void
require_dummynet(atf_tc_t *tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.kmods", "dummynet");
}

ATF_TC(dn_link_short);
ATF_TC_HEAD(dn_link_short, tc)
{
	require_dummynet(tc);
}
ATF_TC_BODY(dn_link_short, tc)
{
	check_short_object(DN_LINK);
}

ATF_TC(dn_profile_short);
ATF_TC_HEAD(dn_profile_short, tc)
{
	require_dummynet(tc);
}
ATF_TC_BODY(dn_profile_short, tc)
{
	check_short_object(DN_PROFILE);
}

ATF_TC(dn_sch_short);
ATF_TC_HEAD(dn_sch_short, tc)
{
	require_dummynet(tc);
}
ATF_TC_BODY(dn_sch_short, tc)
{
	check_short_object(DN_SCH);
}

ATF_TC(dn_fs_short);
ATF_TC_HEAD(dn_fs_short, tc)
{
	require_dummynet(tc);
}
ATF_TC_BODY(dn_fs_short, tc)
{
	check_short_object(DN_FS);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dn_link_short);
	ATF_TP_ADD_TC(tp, dn_profile_short);
	ATF_TP_ADD_TC(tp, dn_sch_short);
	ATF_TP_ADD_TC(tp, dn_fs_short);

	return (atf_no_error());
}
